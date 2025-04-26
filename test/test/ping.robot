*** Variables ***
${LIBRARY_PATH}       ../resources
${PING_BIN}           /ping
${MY_PING_BIN}        /ft_ping
${TEST_ADDRESS}       127.0.0.1
${ICMP_ECHO_REPLY}    0

*** Settings ***
Library            ${LIBRARY_PATH}/TestPingServer.py
Library            Process
Library            String

Test Setup         Start Test Server
Test Teardown      Stop Test Server

*** Keywords ***
Test Blocking Ping
    [Arguments]
    ...                       @{command_arguments}
    ...                       ${count}=3
    ...                       ${icmp_type}=${ICMP_ECHO_REPLY}
    ...                       ${wrong_id}=False

    # Run inetutils ping
    ${process}=               Start Process                 ${PING_BIN}
    ...                       @{command_arguments}          ${TEST_ADDRESS}
    ${messages}=              Wait For Messages
    ...                       count=${count}                comparable=True
    ...                       icmp_type=${icmp_type}
    ...                       wrong_id=${wrong_id}
    Send Signal To Process    SIGINT                        ${process}
    ${result}=                Wait For Process              ${process}
    ${exit_status}=           Set Variable                  ${result.rc}
    ${out}=                   Remove String Using Regexp    ${result.stdout}
    ...                       id 0x[0-9a-f]* = \\d*
    ...                       stddev = \\d+\\.\\d*/\\d+\\.\\d*/\\d+\\.\\d*/\\d+\\.\\d* ms
    Log                       out is: ${out}
    Log                       ${result.stderr}

    # Run ft_ping
    ${process}=               Start Process                 ${MY_PING_BIN}
    ...                       @{command_arguments}          ${TEST_ADDRESS}
    ${my_messages}=           Wait For Messages
    ...                       count=${count}                comparable=True
    ...                       icmp_type=${icmp_type}
    ...                       wrong_id=${wrong_id}
    Send Signal To Process    SIGINT                        ${process}
    ${result}=                Wait For Process              ${process}
    ${my_exit_status}=        Set Variable                  ${result.rc}
    ${my_out}=                Remove String Using Regexp    ${result.stdout}
    ...                       id 0x[0-9a-f]* = \\d*
    ...                       stddev = \\d+\\.\\d*/\\d+\\.\\d*/\\d+\\.\\d*/\\d+\\.\\d* ms
    Log                       my_out is: ${my_out}
    Log                       ${result.stderr}

    # Compare outputs
    Should Be Equal           ${exit_status}                ${my_exit_status}
    Should Be Equal           ${out}                        ${my_out}
    Should Be Equal           ${messages}                   ${my_messages}

Test Non Blocking Ping
    [Arguments]
    ...                   @{command_arguments}
    ...                   ${count}=3
    ...                   ${payload}=
    ...                   ${wrong_checksum}=False

    # Run inetutils ping
    ${process}=           Start Process                 ${PING_BIN}
    ...                   @{command_arguments}          ${TEST_ADDRESS}
    ${messages}=          Wait For Messages             count=${count}           comparable=True
    ...                   payload=${payload}
    ...                   wrong_checksum=${wrong_checksum}
    ${result}=            Wait For Process              ${process}
    ${exit_status}=       Set Variable                  ${result.rc}
    ${out}=               Remove String Using Regexp    ${result.stdout}
    ...                   id 0x[0-9a-f]* = \\d*
    ...                   ttl=\\d*
    ...                   time=\\d+\\.\\d* ms
    ...                   stddev = \\d+\\.\\d*/\\d+\\.\\d*/\\d+\\.\\d*/\\d+\\.\\d* ms
    Log                   out is: ${out}
    Log                   ${result.stderr}

    # Run ft_ping
    ${process}=           Start Process                 ${MY_PING_BIN}
    ...                   @{command_arguments}          ${TEST_ADDRESS}
    ${my_messages}=       Wait For Messages             count=${count}           comparable=True
    ...                   payload=${payload}
    ...                   wrong_checksum=${wrong_checksum}
    ${result}=            Wait For Process              ${process}
    ${my_exit_status}=    Set Variable                  ${result.rc}
    ${my_out}=            Remove String Using Regexp    ${result.stdout}
    ...                   id 0x[0-9a-f]* = \\d*
    ...                   ttl=\\d*
    ...                   time=\\d+\\.\\d* ms
    ...                   stddev = \\d+\\.\\d*/\\d+\\.\\d*/\\d+\\.\\d*/\\d+\\.\\d* ms
    Log                   my_out is: ${my_out}
    Log                   ${result.stderr}

    # Compare outputs
    Should Be Equal       ${exit_status}                ${my_exit_status}
    Should Be Equal       ${out}                        ${my_out}
    Should Be Equal       ${messages}                   ${my_messages}

*** Test Cases ***
Test Receiving
    [Documentation]       Basic send and receive 3 times
    [Timeout]             10s

    Test Non Blocking Ping    -c3

Test Receiving Verbose
    [Documentation]           Basic send and receive 3 times with verbose
    [Timeout]                 10s

    Test Non Blocking Ping    -c3    -v

Test Receiving With Custom Payload
    [Documentation]           Basic send and receive 3 times with a custom payload
    [Timeout]                 10s

    Test Non Blocking Ping    -c3    -v    -pcacadebaca

Test Receiving Wrong Payload
    [Documentation]           Send and receive 3 time with a wrong payload
    [Timeout]                 10s

    Test Non Blocking Ping    -c3    -v    payload=cacadebaca

Test Receiving Wrong Type
    [Documentation]       Send and receive 3 time with a wrong ICMP type
    [Timeout]             10s

    Test Blocking Ping    -c3    -v    icmp_type=42

Test Receiving Wrong Checksum
    [Documentation]           Send and receive 3 time with a wrong checksum
    [Timeout]                 10s

    Test Non Blocking Ping    -c3    -v    wrong_checksum=True

Test Receiving Wrong Id
    [Documentation]       Send and receive 3 time with a wrong Id
    [Timeout]             10s

    Test Blocking Ping    -c3    -v    wrong_id=True
