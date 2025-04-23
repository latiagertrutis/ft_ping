*** Variables ***

${LIBRARY_PATH}    ../resources
${PING_BIN}        /ft_ping

*** Settings ***
Library            ${LIBRARY_PATH}/TestPingServer.py
Library            Process

Test Setup         Start Test Server
Test Teardown      Stop Test Server

*** Test Cases ***
Test Receiving
    [Documentation]    Setup and teardown from setting section
    [Timeout]           10s
    ${process}=    Start Process       ${PING_BIN}    -c1    127.0.0.1
    Wait For Message
    Reply Message
    ${result}=     Wait For Process    ${process}
    Log            Stdout is: ${result.stdout}
    Log            Stderr is: ${result.stderr}
