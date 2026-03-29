#!/bin/bash

# Configuration
BUILD_DIR="build"
SERVER_BIN="./build/bin/exchange_sim"
CLIENT_BIN="./build/feed_processor"

# Colors for terminal output
GREEN='\033[0;32m'
BLUE='\033[0;34m'
RED='\033[0;31m'
NC='\033[0m' # No Color

function build_project() {
    echo -e "${BLUE}[1/3] Initializing Build...${NC}"
    mkdir -p $BUILD_DIR
    cd $BUILD_DIR
    cmake ..
    make -j$(nproc)
    if [ $? -eq 0 ]; then
        echo -e "${GREEN}Build Successful!${NC}"
    else
        echo -e "${RED}Build Failed!${NC}"
        exit 1
    fi
    cd ..
}

function run_server() {
    echo -e "${GREEN}Starting Exchange Simulator...${NC}"
    $SERVER_BIN
}

function run_client() {
    echo -e "${BLUE}Starting HFT Client...${NC}"
    $CLIENT_BIN
}

function run_demo() {
    echo -e "${GREEN}Launching Demo Mode in separate terminals...${NC}"

    # 1. Start Server in a new terminal
    
    gnome-terminal --geometry=80x24+0+0 --title="EXCHANGE_SERVER" -- bash -c "echo 'STARTING SERVER...'; $SERVER_BIN; echo 'Server stopped.'; exec bash" &
    
    sleep 2
    
    # 2. Start Client in a new terminal
    gnome-terminal --geometry=80x24+500+0 --title="HFT_CLIENT" -- bash -c "echo 'STARTING CLIENT...'; $CLIENT_BIN; echo 'Client stopped.'; exec bash"

    echo -e "${GREEN}Both applications are now running in separate windows.${NC}"
}

# Command Router
case "$1" in
    build)
        build_project
        ;;
    server)
        run_server
        ;;
    client)
        run_client
        ;;
    demo)
        build_project
        run_demo
        ;;
    *)
        echo "Usage: $0 {build|server|client|demo}"
        echo "  build  : Compiles the project using CMake"
        echo "  server : Runs only the Exchange Simulator"
        echo "  client : Runs only the HFT Client"
        echo "  demo   : Builds and runs both together"
        exit 1
esac