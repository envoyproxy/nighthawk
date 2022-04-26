use("github.com/repokitteh/modules/assign.star")
use("github.com/repokitteh/modules/review.star")
use("github.com/repokitteh/modules/wait.star")
use("github.com/envoyproxy/nighthawk/ci/repokitteh/modules/azure_pipelines.star", secret_token = get_secret("azp_token"))

alias("retest", "retry-azp")
