use toolshed_echo::runner::main as echo_main;

#[tokio::main]
async fn main() {
    echo_main().await
}
