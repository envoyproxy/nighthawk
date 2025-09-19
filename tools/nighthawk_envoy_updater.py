#!/usr/bin/env python3
"""Updates the Envoy dependency in the Nighthawk repository.

This script automates the process of updating the Envoy commit hash in the
Nighthawk project. It finds the latest Envoy commit that passes Nighthawk's
tests, and if successful, creates a pull request with the update. If there is
a non-trivial Envoy merge, it leaves the local Nighthawk git repo in the
simplest compromised state and prompts the user to resolve merge conflicts,
rebase, and re-run the utility to proceed.
"""

import argparse
import datetime
import enum
import pathlib
import pprint
import re
import shlex
import subprocess
import sys
import tempfile
from typing import Generic, TypeVar

MAX_AGENT_ATTEMPTS = 3

# Files Nighthawk re-uses (almost) verbatim from its Envoy dependency.
shared_files: list[str] = [
    ".bazelrc",
    ".bazelversion",
    "ci/docker-compose.yml",
    "ci/run_envoy_docker.sh",
    "tools/code_format/config.yaml",
    "tools/gen_compilation_database.py",
]


def _run_command(
    command: list[str],
    cwd: pathlib.Path | None = None,
    input_str: str | None = None,
    shell: bool = False,
    interactive: bool = False,
) -> str:
  """Run a shell command and return the standard output."""
  if interactive:
    print("\nThe current operation may require human attention to proceed.")
    input("Press enter and monitor for ~10 seconds for more prompts...")
  cmd = (f'pushd {cwd if cwd else pathlib.Path.cwd()} && {" ".join(command)} &&'
         " popd")
  print(f"$ {cmd}")
  try:
    result = subprocess.run(
        command,
        cwd=cwd,
        input=input_str,
        shell=shell,
        capture_output=True,
        text=True,
        check=True,
    )
    return result.stdout.strip() if result.stdout else ""
  except subprocess.CalledProcessError as e:
    print(e, file=sys.stderr)
    # Update the captured command to include the subprocess's working directory.
    setattr(e, "cmd", cmd)
    raise


def _run_sed_replace(old_pattern: str, new_pattern: str, filename: str):
  changelog = _run_command(
      [f"sed -i 's/{old_pattern}/{new_pattern}/w /dev/stdout' {filename}"],
      shell=True,
  )
  if changelog != new_pattern:
    raise RuntimeError("Failed to replace file contents.")


def _non_trivial_merge_instructions(envoy_commit: str, nighthawk_git_repo_dir: str,
                                    branch_name: str):
  return f"""Ran partial integration of Envoy commit {envoy_commit}.

      The Nighthawk repository branch {branch_name} has been left in a state
      with the Envoy commit {envoy_commit} integration applied. This integration
      introduced merge conflicts, returned tooling errors, or failed to build
      and pass its tests. Please see error logs upstream for details.

      Please address any merge conflicts, tooling failures, and build or test
      failures, then commit your manual changes using:

        git -C {nighthawk_git_repo_dir} add . && \
          git -C {nighthawk_git_repo_dir} commit -m 'Updating Envoy to {envoy_commit}'

      Link to failing commit:
      https://github.com/envoyproxy/envoy/commit/{envoy_commit}
      """


def _patch_merge_conflict_instructions(envoy_commit: str, nighthawk_git_repo_dir: str):
  return f"""New merge conflicts from integrating changes in shared files.

      Nighthawk maintains separate copies of a subset of files from the Envoy
      repository: {", ".join(shared_files)}

      These files are used for configuring the automated build and test
      environments and should be kept in-sync between the two repos. Nighthawk's
      copies contain small modifications necessary for the testing environment
      and marked inline with `# unique`.

      Integrating changes in shared files from commit {envoy_commit}
      introduced merge conflicts because Envoy modified a lined marked by
      `# unique`. These merge conflicts are recorded in the {nighthawk_git_repo_dir} repo
      as ".rej" files.

      For merge conflicts, you must manually inspect Nighthawk's copy and
      combine the Envoy changes with the old Nighthawk version into an updated
      Nighthawk-specific "# unique" line.

      Merge conflict markers `<<<<<<< ======= >>>>>>>` have been inserted
      directly in Nighthawk's copy of the files. You can use a graphical
      three-way merge tool such as code or follow instructions at
      https://docs.github.com/en/pull-requests/collaborating-with-pull-requests/addressing-merge-conflicts/resolving-a-merge-conflict-using-the-command-line?platform=linux
      """


class StepStatus(enum.Enum):
  """The status of a step in the process."""

  PENDING = enum.auto()
  SUCCESS = enum.auto()
  FAILED = enum.auto()
  CANCELLED = enum.auto()
  NOT_PLANNED = enum.auto()


TStep = TypeVar("TStep", bound=enum.Enum)


class StepHandler(Generic[TStep]):
  """Base class for handling a sequence of steps.

  Attributes:
    step_enum: The enum type for the steps.
    steps: A list of all possible steps.
    step_tracker: A dictionary to track the status and errors of each step.
    agent_invocation: Command template to invoke an LLM agent to fix step
      failures.
  """

  def __init__(self, step_enum: TStep, agent_invocation: str | None = None):
    """Initialize the StepHandler.

    Args:
      step_enum: The enum type defining the sequence of steps.
      agent_invocation: Command template to invoke the agent.
    """
    self.steps = list(step_enum)
    self.step_tracker = {step: StepStatus.PENDING for step in self.steps}
    self.agent_invocation = agent_invocation

  def _set_step_success(self, step: TStep):
    """Set the status of a step to SUCCESS."""
    self.step_tracker[step] = StepStatus.SUCCESS

  def _set_step_failure(self, step: TStep, error: str):
    """Set the status of a step to FAILED and record the error."""
    self.step_tracker[step] = StepStatus.FAILED
    for member in self.steps:
      if self.step_tracker[member] == StepStatus.PENDING:
        self.step_tracker[step] = StepStatus.CANCELLED

  def _set_step_not_planned(self, step: TStep):
    """Set the status of a step to NOT_PLANNED."""
    self.step_tracker[step] = StepStatus.NOT_PLANNED

  def _run_step(self, step: TStep):
    """Run the logic for a single step. Must be overridden by subclasses."""
    raise RuntimeError("Must be overridden")

  def _get_step_failure_summary(self, step: TStep, error: Exception) -> str:
    """Create a readable and understandable summary of a step execution failure."""
    stdout = getattr(error, "stdout", "N/A")
    stderr = getattr(error, "stderr", "N/A")
    cmd = getattr(error, "cmd", "N/A")

    return "\n".join([
        "## Failing Step Context\n",
        f"- **Failing Step:** {step.name}",
        f"- **Error:** {error}",
        f"- **Failed Command:** `{cmd}`",
        "",
        "### STDOUT\n",
        f"```\n{stdout}\n```",
        "",
        "### STDERR\n",
        f"```\n{stderr}\n```",
        "",
        "## StepHandler Instance State\n",
        pprint.pformat(vars(self), sort_dicts=False),
        "",
    ])

  def _get_additional_agent_context(self, step: TStep, error: Exception) -> str:
    """Return additional context for the agent prompt. Can be overridden."""
    return ""

  def _create_agent_prompt(self, step: TStep, error: Exception) -> str:
    """Create a prompt file for the agent and return the path."""
    cmd = getattr(error, "cmd", "N/A")

    failure_summary = self._get_step_failure_summary(step, error)
    additional_context = self._get_additional_agent_context(step, error)
    instructions = "\n".join([
        "## Agent Instructions\n",
        "Based on the context and instance state above, fix the files in the ",
        "nighthawk_git_repo_dir to resolve the error. The primary goal ",
        "is to make the failing command pass:",
        f"```sh\n{cmd}\n```",
        "Avoid changing the behavior of Nighthawk's tests, if possible.",
        "The Envoy source code for the target commit is available in a tmp ",
        "directory under the Nighthawk project root parent, envoy_git_repo_dir.",
    ])
    return "\n".join([failure_summary, additional_context, instructions])

  def _run_steps(self, line_prefix: str, prior_attempts=0) -> bool:
    """Run all pending steps in sequence.

    Args:
      line_prefix: A string to prefix to each log line.

    Returns:
      True if all steps completed successfully, False otherwise.
    """
    clean_finish = True
    for step in self.steps:
      if self.step_tracker[step] != StepStatus.PENDING:
        continue
      print(f"\n{line_prefix}{step}")
      try:
        self._run_step(step)
        self._set_step_success(step)
      except (
          RuntimeError,
          FileNotFoundError,
          ValueError,
          subprocess.CalledProcessError,
      ) as e:
        if not self.agent_invocation or prior_attempts >= MAX_AGENT_ATTEMPTS:
          self._set_step_failure(step, e)
          clean_finish = False
          print(self._get_step_failure_summary(step, e))
        else:
          print(f"\n\nStep {step.name} failed. Invoking agent (attempt"
                f" {prior_attempts + 1}/{MAX_AGENT_ATTEMPTS})...")
          prompt = self._create_agent_prompt(step, e)
          print(f"\n\nAgent prompt:\n\n{prompt}\n\n")
          subprocess.run(
              shlex.split(self.agent_invocation),
              cwd=self.nighthawk_git_repo_dir.parent
              if hasattr(self, "nighthawk_git_repo_dir") else None,
              input=prompt,
              text=True,
              check=True,
          )
          print("Agent fix attempt finished. Retrying...")
          self._run_steps(line_prefix, prior_attempts + 1)
    return clean_finish


class EnvoyCommitIntegrationStep(enum.Enum):
  """Steps to integrate a specific Envoy commit into Nighthawk."""

  RESET_NIGHTHAWK_REPO = enum.auto()
  GET_ENVOY_SHA = enum.auto()
  SET_NIGHTHAWK_BAZEL_DEP = enum.auto()
  PATCH_SHARED_FILES = enum.auto()
  BAZEL_UPDATE_REQUIREMENTS = enum.auto()
  BUILD_NIGHTHAWK = enum.auto()
  TEST_NIGHTHAWK = enum.auto()
  UPDATE_CLI_README = enum.auto()
  FIX_FORMAT = enum.auto()


class EnvoyCommitIntegration(StepHandler[EnvoyCommitIntegrationStep]):
  """Handles the steps to integrate a specific Envoy commit into Nighthawk."""

  def __init__(
      self,
      nighthawk_git_repo_dir: pathlib.Path,
      envoy_git_repo_dir: pathlib.Path,
      current_envoy_commit: str,
      target_envoy_commit: str,
      agent_invocation: str,
  ):
    """Initialize the EnvoyCommitIntegration.

    Args:
      nighthawk_git_repo_dir: The path to the local Nighthawk git repository.
      envoy_git_repo_dir: The path to the local Envoy git repository.
      current_envoy_commit: The current Envoy commit hash in Nighthawk.
      target_envoy_commit: The target Envoy commit hash to integrate.
      agent_invocation: Command template to invoke an LLM agent to fix step
        failures.
    """
    super().__init__(EnvoyCommitIntegrationStep, agent_invocation)
    self.nighthawk_git_repo_dir = nighthawk_git_repo_dir
    self.envoy_git_repo_dir = envoy_git_repo_dir
    self.current_envoy_commit = current_envoy_commit
    self.target_envoy_commit = target_envoy_commit
    self.envoy_sha = None

  def _get_additional_agent_context(self, step: EnvoyCommitIntegrationStep,
                                    error: Exception) -> str:
    """Provide the git repo diff information as additional context for the agent."""
    try:
      nighthawk_diff = _run_command(["git", "diff", "--no-color", "-U1"],
                                    cwd=self.nighthawk_git_repo_dir)
      envoy_diff_stat = _run_command([
          "git", "diff", "--no-color", "--stat",
          f"{self.current_envoy_commit}..{self.target_envoy_commit}"
      ],
                                     cwd=self.envoy_git_repo_dir)
      return "\n".join([
          "## Nighthawk diff\n",
          "The Nighthawk git repo has already been modified, per the following diff:",
          f"```diff\n{nighthawk_diff}\n```",
          "",
          "## Envoy Diff (current to target)\n",
          "The following diff stat describes how Envoy has been modified between ",
          "Nighthawk's current Envoy commit and its new target Envoy commit:",
          f"```diff\n{envoy_diff_stat}\n```",
          "",
      ])
    except Exception:
      return ""

  def _run_step(self, step: EnvoyCommitIntegrationStep):
    """Run the logic for a single Envoy commit integration step."""
    match step:
      case EnvoyCommitIntegrationStep.RESET_NIGHTHAWK_REPO:
        _run_command(["git", "reset", "--hard", "HEAD"], cwd=self.nighthawk_git_repo_dir)
        _run_command(["git", "clean", "-fdx"], cwd=self.nighthawk_git_repo_dir)
      case EnvoyCommitIntegrationStep.GET_ENVOY_SHA:
        tarball_url = f"https://github.com/envoyproxy/envoy/archive/{self.target_envoy_commit}.tar.gz"
        self.envoy_sha = _run_command(
            [f"curl -sL {tarball_url} | sha256sum | cut -d ' ' -f 1"],
            shell=True,
        )
      case EnvoyCommitIntegrationStep.SET_NIGHTHAWK_BAZEL_DEP:
        repo_file = self.nighthawk_git_repo_dir / "bazel/repositories.bzl"
        _run_sed_replace(
            old_pattern='ENVOY_COMMIT = ".*"',
            new_pattern=f'ENVOY_COMMIT = "{self.target_envoy_commit}"',
            filename=str(repo_file),
        )
        _run_sed_replace(
            old_pattern='ENVOY_SHA = ".*"',
            new_pattern=f'ENVOY_SHA = "{self.envoy_sha}"',
            filename=str(repo_file),
        )
      case EnvoyCommitIntegrationStep.PATCH_SHARED_FILES:
        patch_file_name = _run_command(["mktemp"])
        _run_command(
            [
                "git diff"
                f" {self.current_envoy_commit}..{self.target_envoy_commit} -- " +
                " ".join(shared_files) + f" > {patch_file_name}"
            ],
            cwd=self.envoy_git_repo_dir,
            shell=True,
        )

        if pathlib.Path(patch_file_name).stat().st_size == 0:
          # No changes for shared files in Envoy diff.
          return

        try:
          _run_command(
              ["git apply --ignore-whitespace --ignore-space-change"
               f" < {patch_file_name}"],
              cwd=self.nighthawk_git_repo_dir,
              shell=True,
          )
        except subprocess.CalledProcessError as e:
          _run_command(
              [
                  "git apply --reject --ignore-whitespace --ignore-space-change"
                  f" < {patch_file_name}"
              ],
              cwd=self.nighthawk_git_repo_dir,
              shell=True,
          )
          raise RuntimeError(
              _patch_merge_conflict_instructions(self.target_envoy_commit,
                                                 self.nighthawk_git_repo_dir)) from e
      case EnvoyCommitIntegrationStep.BAZEL_UPDATE_REQUIREMENTS:
        _run_command(
            ["bazel", "run", "//tools/base:requirements.update"],
            cwd=self.nighthawk_git_repo_dir,
        )
      case EnvoyCommitIntegrationStep.BUILD_NIGHTHAWK:
        _run_command(["./ci/do_ci.sh", "build"], cwd=self.nighthawk_git_repo_dir)
      case EnvoyCommitIntegrationStep.TEST_NIGHTHAWK:
        _run_command(["./ci/do_ci.sh", "test"], cwd=self.nighthawk_git_repo_dir)
      case EnvoyCommitIntegrationStep.UPDATE_CLI_README:
        _run_command(
            ["./tools/update_cli_readme_documentation.sh", "--mode=fix"],
            cwd=self.nighthawk_git_repo_dir,
        )
      case EnvoyCommitIntegrationStep.FIX_FORMAT:
        _run_command(["./ci/do_ci.sh", "fix_format"], cwd=self.nighthawk_git_repo_dir)
      case _:
        raise ValueError(f"{step} is not supported.")

  def run_envoy_commit_integration_steps(self) -> bool:
    """Run all steps for integrating the target Envoy commit."""
    return self._run_steps(line_prefix="....EnvoyCommitIntegration: ")


class NighthawkEnvoyUpdateStep(enum.Enum):
  """Overall steps for updating Envoy dependency in Nighthawk."""

  CHECK_NIGHTHAWK_DIR = enum.auto()
  CHECK_NIGHTHAWK_GIT_REPO = enum.auto()
  CHECK_NIGHTHAWK_GIT_SIGNING = enum.auto()
  CHECK_NIGHTHAWK_GIT_STATUS = enum.auto()
  CHECK_NIGHTHAWK_UPSTREAM_REMOTE = enum.auto()
  CHECK_NIGHTHAWK_ORIGIN_REMOTE = enum.auto()
  SYNC_NIGHTHAWK_REPO = enum.auto()
  CHECKOUT_UPDATE_BRANCH = enum.auto()
  GET_CURRENT_ENVOY_COMMIT = enum.auto()
  CLONE_ENVOY = enum.auto()
  CHECK_ENVOY_CLONE_DEPTH = enum.auto()
  GET_LATEST_ENVOY_COMMIT = enum.auto()
  GET_ENVOY_COMMIT_RANGE = enum.auto()
  BAZEL_CLEAN_EXPUNGE = enum.auto()
  FIND_LATEST_TRIVIAL_MERGE = enum.auto()
  COMMIT_AND_PUSH_UPDATE_BRANCH = enum.auto()
  APPLY_PARTIAL_NON_TRIVIAL_MERGE = enum.auto()


class NighthawkEnvoyUpdate(StepHandler[NighthawkEnvoyUpdateStep]):
  """Handles the steps to update the Envoy dependency in Nighthawk."""

  def __init__(
      self,
      nighthawk_git_repo_dir: pathlib.Path,
      branch_name: str,
      envoy_clone_depth: int,
      sync_nighthawk_repo: bool,
      agent_invocation: str | None = None,
  ):
    """Initialize the NighthawkEnvoyUpdate.

    Args:
      nighthawk_git_repo_dir: The path to the local Nighthawk git repository.
      branch_name: The name of the branch to create for the update.
      sync_nighthawk_repo: Whether to sync the Nighthawk repo with upstream.
      envoy_clone_depth: The depth to use when cloning the Envoy repository.
      agent_invocation: Command template to invoke an LLM agent to fix step
        failures.
    """
    super().__init__(NighthawkEnvoyUpdateStep, agent_invocation)
    self.nighthawk_git_repo_dir = nighthawk_git_repo_dir.expanduser()
    self.branch_name = branch_name
    self.envoy_clone_depth = envoy_clone_depth

    self._envoy_tmp_dir = tempfile.TemporaryDirectory(dir=self.nighthawk_git_repo_dir.parent,
                                                      prefix="envoy-clone-")
    self.envoy_git_repo_dir = pathlib.Path(self._envoy_tmp_dir.name)

    self.github_username = None
    self.current_envoy_commit = None
    self.latest_envoy_commit = None
    self.envoy_commits_current_to_latest = None
    self.best_envoy_commit = None
    self.first_non_trivial_commit = None

    # The user can disable syncing the Nighthawk repo.
    if not sync_nighthawk_repo:
      self._set_step_not_planned(NighthawkEnvoyUpdateStep.SYNC_NIGHTHAWK_REPO)

  def cleanup(self):
    """Clean up state modifications made by NighthawkEnvoyUpdate."""
    self._envoy_tmp_dir.cleanup()

  def _run_step(self, step: NighthawkEnvoyUpdateStep):
    """Run the logic for a single Nighthawk Envoy update step."""
    match step:
      case NighthawkEnvoyUpdateStep.CHECK_NIGHTHAWK_DIR:
        if not self.nighthawk_git_repo_dir.is_dir():
          raise RuntimeError(f"Nighthawk directory not found: {self.nighthawk_git_repo_dir}")
      case NighthawkEnvoyUpdateStep.CHECK_NIGHTHAWK_GIT_REPO:
        try:
          _run_command(
              ["git", "rev-parse", "--is-inside-work-tree"],
              cwd=self.nighthawk_git_repo_dir,
          )
        except subprocess.CalledProcessError as e:
          raise RuntimeError(f"Nighthawk directory {self.nighthawk_git_repo_dir} is not a git"
                             " repository.") from e
      case NighthawkEnvoyUpdateStep.CHECK_NIGHTHAWK_GIT_SIGNING:
        try:
          _run_command(
              [
                  "cmp",
                  "-s",
                  "support/hooks/prepare-commit-msg",
                  ".git/hooks/prepare-commit-msg",
              ],
              cwd=self.nighthawk_git_repo_dir,
          )
        except subprocess.CalledProcessError as e:
          raise RuntimeError(f"Nighthawk directory {self.nighthawk_git_repo_dir} is not"
                             " configured to use a signing key with commits.") from e
      case NighthawkEnvoyUpdateStep.CHECK_NIGHTHAWK_GIT_STATUS:
        if _run_command(["git", "status", "--porcelain"], cwd=self.nighthawk_git_repo_dir):
          raise RuntimeError("Nighthawk has uncommitted changes. Please reset or commit them.")
      case NighthawkEnvoyUpdateStep.CHECK_NIGHTHAWK_UPSTREAM_REMOTE:
        expected_upstream = "https://github.com/envoyproxy/nighthawk"
        try:
          upstream_url = _run_command(
              ["git", "remote", "get-url", "upstream"],
              cwd=self.nighthawk_git_repo_dir,
          )
          if upstream_url != expected_upstream:
            raise RuntimeError(f"Nighthawk directory {self.nighthawk_git_repo_dir} remote"
                               f" upstream is not set to {expected_upstream}. Got:"
                               f" {upstream_url}")
        except subprocess.CalledProcessError as e:
          raise RuntimeError(f"Nighthawk directory {self.nighthawk_git_repo_dir} does not have"
                             f" a remote named upstream set to {expected_upstream}.") from e
      case NighthawkEnvoyUpdateStep.CHECK_NIGHTHAWK_ORIGIN_REMOTE:
        try:
          origin_url = _run_command(
              ["git", "remote", "get-url", "origin"],
              cwd=self.nighthawk_git_repo_dir,
          )
        except subprocess.CalledProcessError as e:
          raise RuntimeError("Failed to get remote origin URL. Is"
                             f" {self.nighthawk_git_repo_dir} a git repository with an origin"
                             " remote configured?") from e
        match = re.search(r"github.com[:/](.*?)/nighthawk(?:.git)?$", origin_url)
        if match:
          self.github_username = match.group(1)
        else:
          raise RuntimeError(f"Could not extract GitHub username from origin URL: {origin_url}")
      case NighthawkEnvoyUpdateStep.SYNC_NIGHTHAWK_REPO:
        _run_command(["git", "checkout", "main"], cwd=self.nighthawk_git_repo_dir)
        _run_command(
            ["git fetch --all && git merge upstream/main && git push origin"
             " main"],
            cwd=self.nighthawk_git_repo_dir,
            interactive=True,
            shell=True,
        )
      case NighthawkEnvoyUpdateStep.CHECKOUT_UPDATE_BRANCH:
        try:
          _run_command(
              ["git", "checkout", "-b", self.branch_name, "origin/main"],
              cwd=self.nighthawk_git_repo_dir,
          )
        except subprocess.CalledProcessError:
          # Failed to create branch, attempting to checkout.
          _run_command(
              ["git", "checkout", self.branch_name],
              cwd=self.nighthawk_git_repo_dir,
          )
          _run_command(["git", "rebase", "origin/main"], cwd=self.nighthawk_git_repo_dir)
      case NighthawkEnvoyUpdateStep.GET_CURRENT_ENVOY_COMMIT:
        repo_file = self.nighthawk_git_repo_dir / "bazel/repositories.bzl"
        self.current_envoy_commit = _run_command(
            [r"""sed -nE 's/^ENVOY_COMMIT = "(.*)"$/\1/p' """ + str(repo_file)],
            shell=True,
        )
        if (not self.current_envoy_commit or len(self.current_envoy_commit) != 40):
          raise RuntimeError(f"Failed to extract current Envoy commit from {repo_file}")
      case NighthawkEnvoyUpdateStep.CLONE_ENVOY:
        _run_command([
            "git",
            "clone",
            f"--depth={self.envoy_clone_depth}",
            "https://github.com/envoyproxy/envoy.git",
            str(self.envoy_git_repo_dir),
        ],)
      case NighthawkEnvoyUpdateStep.CHECK_ENVOY_CLONE_DEPTH:
        try:
          _run_command(
              ["git", "cat-file", "-e", self.current_envoy_commit],
              cwd=self.envoy_git_repo_dir,
          )
        except subprocess.CalledProcessError as e:
          raise RuntimeError(f"The current Envoy commit {self.current_envoy_commit} used in"
                             " Nighthawk was not found in the cloned Envoy repository"
                             " history. Please try again with a larger --envoy_clone_depth"
                             f" value than {self.envoy_clone_depth}.") from e
      case NighthawkEnvoyUpdateStep.GET_LATEST_ENVOY_COMMIT:
        self.latest_envoy_commit = _run_command(["git", "rev-parse", "main"],
                                                cwd=self.envoy_git_repo_dir)
        if self.current_envoy_commit == self.latest_envoy_commit:
          print("\n\nNighthawk is up-to-date, no new commits in Envoy repo.")
          sys.exit(0)
      case NighthawkEnvoyUpdateStep.GET_ENVOY_COMMIT_RANGE:
        current_to_latest_raw = _run_command(
            [
                "git",
                "log",
                "--reverse",
                "--pretty=%H",
                f"{self.current_envoy_commit}..{self.latest_envoy_commit}",
            ],
            cwd=self.envoy_git_repo_dir,
        )

        self.envoy_commits_current_to_latest = [c for c in current_to_latest_raw.splitlines() if c]
      case NighthawkEnvoyUpdateStep.BAZEL_CLEAN_EXPUNGE:
        _run_command(["bazel", "clean", "--expunge"], cwd=self.nighthawk_git_repo_dir)
      case NighthawkEnvoyUpdateStep.FIND_LATEST_TRIVIAL_MERGE:
        statuses = [" " * 8] * len(self.envoy_commits_current_to_latest)

        low = 0
        high = len(self.envoy_commits_current_to_latest) - 1
        latest_passing_commit_index = -1

        # Start by testing the latest commit
        index_to_test = high

        while low <= high:
          target_envoy_commit = self.envoy_commits_current_to_latest[index_to_test]
          statuses[index_to_test] = "----->"

          print("\nBisection status:")
          for i, commit in enumerate(self.envoy_commits_current_to_latest):
            status = statuses[i]
            if (status.strip() or i == 0 or i == len(self.envoy_commits_current_to_latest) - 1):
              print(f"[{status:^8}]"
                    f" https://github.com/envoyproxy/envoy/commit/{commit}")
          print("")

          clean_integration = EnvoyCommitIntegration(
              nighthawk_git_repo_dir=self.nighthawk_git_repo_dir,
              envoy_git_repo_dir=self.envoy_git_repo_dir,
              current_envoy_commit=self.current_envoy_commit,
              target_envoy_commit=target_envoy_commit,
              agent_invocation=self.agent_invocation,
          ).run_envoy_commit_integration_steps()

          if clean_integration:
            statuses[index_to_test] = "PASSED"
            latest_passing_commit_index = index_to_test
            low = index_to_test + 1
          else:
            statuses[index_to_test] = "FAILED"
            high = index_to_test - 1

          index_to_test = low + (high - low) // 2

        if latest_passing_commit_index == -1:
          # Bisecting failed to find an Envoy commit that can be trivially
          # integrated.
          self.best_envoy_commit = None
          self.first_non_trivial_commit = self.envoy_commits_current_to_latest[0]
          self._set_step_not_planned(NighthawkEnvoyUpdateStep.COMMIT_AND_PUSH_UPDATE_BRANCH)
        elif (latest_passing_commit_index == len(self.envoy_commits_current_to_latest) - 1):
          # The latest Envoy commit can be trivially integrated.
          self.best_envoy_commit = self.envoy_commits_current_to_latest[-1]
          self.first_non_trivial_commit = None
          self._set_step_not_planned(NighthawkEnvoyUpdateStep.APPLY_PARTIAL_NON_TRIVIAL_MERGE)
        else:
          # A trivially integrated Envoy commit was found and there are further
          # Envoy commits after it.
          self.first_non_trivial_commit = self.envoy_commits_current_to_latest[
              latest_passing_commit_index + 1]

          # Set the Nighthawk repo to the latest best commit
          self.best_envoy_commit = self.envoy_commits_current_to_latest[latest_passing_commit_index]
          EnvoyCommitIntegration(
              nighthawk_git_repo_dir=self.nighthawk_git_repo_dir,
              envoy_git_repo_dir=self.envoy_git_repo_dir,
              current_envoy_commit=self.current_envoy_commit,
              target_envoy_commit=self.best_envoy_commit,
              agent_invocation=self.agent_invocation,
          ).run_envoy_commit_integration_steps()
      case NighthawkEnvoyUpdateStep.COMMIT_AND_PUSH_UPDATE_BRANCH:
        if not self.best_envoy_commit:
          raise RuntimeError("Nighthawk repo attempting to commit when no trivial Envoy"
                             " merges were found.")
        _run_command(["git", "add", "."], cwd=self.nighthawk_git_repo_dir)
        _run_command(
            [
                "git",
                "commit",
                "-m",
                ("Updating Envoy version to"
                 f" {self.best_envoy_commit[:7]} ({datetime.datetime.now().strftime('%b %d, %Y')})"
                ),
            ],
            cwd=self.nighthawk_git_repo_dir,
            interactive=True,
        )
        _run_command(
            [
                "git",
                "push",
                "--force",
                "--set-upstream",
                "origin",
                self.branch_name,
            ],
            cwd=self.nighthawk_git_repo_dir,
            interactive=True,
        )
        print(f"""
            Please create a pull request on GitHub:
              https://github.com/{self.github_username}/nighthawk/pull/new/{self.branch_name}
            """)
      case NighthawkEnvoyUpdateStep.APPLY_PARTIAL_NON_TRIVIAL_MERGE:
        if not self.first_non_trivial_commit:
          raise RuntimeError("Nighthawk repo attempting to apply partial merge when"
                             " first_non_trivial_commit is not set.")
        EnvoyCommitIntegration(
            nighthawk_git_repo_dir=self.nighthawk_git_repo_dir,
            envoy_git_repo_dir=self.envoy_git_repo_dir,
            current_envoy_commit=self.current_envoy_commit,
            target_envoy_commit=self.first_non_trivial_commit,
            agent_invocation=self.agent_invocation,
        ).run_envoy_commit_integration_steps()
        raise RuntimeError(
            _non_trivial_merge_instructions(
                envoy_commit=self.first_non_trivial_commit,
                nighthawk_git_repo_dir=self.nighthawk_git_repo_dir,
                branch_name=self.branch_name,
            ))
      case _:
        raise ValueError(f"{step} is not supported.")

  def run_update(self) -> bool:
    """Run all steps for the Nighthawk Envoy update process."""
    self._run_steps(line_prefix="NighthawkEnvoyUpdate: ")


def main() -> None:
  """Update Envoy version in Nighthawk."""
  parser = argparse.ArgumentParser(description="Update Envoy version in Nighthawk.")
  parser.add_argument(
      "--nighthawk_dir",
      type=pathlib.Path,
      default="~/github/nighthawk",
      help="The path to the local Nighthawk git repository clone.",
  )
  parser.add_argument(
      "--envoy_clone_depth",
      type=int,
      default=200,
      help="The depth to use when cloning the Envoy repository.",
  )
  parser.add_argument(
      "--branch_name",
      type=str,
      default=f"update-envoy-{datetime.datetime.now().strftime('%Y%m%d')}",
      help=("The name of the branch to create in the Nighthawk repository for the"
            " update."),
  )
  parser.add_argument(
      "--no_sync_nighthawk_repo",
      action="store_false",
      dest="sync_nighthawk_repo",
      help=("If set, the script will not sync the local Nighthawk repository with"
            " the upstream remote before starting the update process."),
  )
  parser.add_argument(
      "--agent_invocation",
      type=str,
      default=None,
      help=("Command to invoke an LLM agent, e.g. 'gemini', with prompt by stdin."),
  )

  args = parser.parse_args()

  updater = NighthawkEnvoyUpdate(
      nighthawk_git_repo_dir=args.nighthawk_dir,
      branch_name=args.branch_name,
      envoy_clone_depth=args.envoy_clone_depth,
      sync_nighthawk_repo=args.sync_nighthawk_repo,
      agent_invocation=args.agent_invocation,
  )
  try:
    updater.run_update()
  finally:
    updater.cleanup()


if __name__ == "__main__":
  main()
