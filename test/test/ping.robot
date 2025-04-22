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
    Run Process        ${PING_BIN}    127.0.0.1    timeout=10secs
    Wait For Message
