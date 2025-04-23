*** Variables ***
${LIBRARY_PATH}    ../resources
${PING_BIN}        /ft_ping
${TEST_ADDRESS}    127.0.0.1

*** Settings ***
Library            ${LIBRARY_PATH}/TestPingServer.py
Library            Process

Test Setup         Start Test Server
Test Teardown      Stop Test Server

*** Test Cases ***
Test Receiving
    [Documentation]      Basic send and receive 3 times
    [Timeout]            10s
    ${process}=          Start Process       ${PING_BIN}    -c3    ${TEST_ADDRESS}
    Wait For Messages    count=3
    ${result}=           Wait For Process    ${process}
    Log                  Stdout is: ${result.stdout}
    Log                  Stderr is: ${result.stderr}
