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

*** Keywords ***
Test Blocking Ping
    [Arguments]
    ...                       @{command_arguments}
    ...                       ${count}=${3}
    ...                       ${icmp_type}=${ICMP_ECHO_REPLY}
    ...                       ${wrong_id}=False

    Start Test Server
    ${process}=               Start Process       @{command_arguments}
    ${messages}=              Wait For Messages
    ...                       count=${count}      comparable=True
    ...                       icmp_type=${icmp_type}
    ...                       wrong_id=${wrong_id}
    Send Signal To Process    SIGINT              ${process}
    ${result}=                Wait For Process    ${process}
    Stop Test Server
    RETURN                    ${result}           ${messages}

Test Non Blocking Ping
    [Arguments]
    ...             @{command_arguments}
    ...             ${count}=${3}
    ...             ${payload}=
    ...             ${wrong_checksum}=False

    Start Test Server
    ${process}=     Start Process        @{command_arguments}
    ${messages}=    Wait For Messages    count=${count}    comparable=True
    ...             payload=${payload}
    ...             wrong_checksum=${wrong_checksum}
    ${result}=      Wait For Process     ${process}
    Stop Test Server
    RETURN          ${result}            ${messages}

Trim Ping Output
    [Arguments]       ${out}
    ${trimmed_out}    Remove String Using Regexp    ${out}
    ...               id 0x[0-9a-f]* = \\d*
    ...               ttl=\\d*
    ...               time=\\d+\\.\\d* ms
    ...               stddev = \\d+\\.\\d*/\\d+\\.\\d*/\\d+\\.\\d*/\\d+\\.\\d* ms

    RETURN            ${trimmed_out}

Compare Ping Outputs
    [Arguments]
    ...                ${exit_status}    ${my_exit_status}
    ...                ${out}            ${my_out}
    ...                ${messages}       ${my_messages}

    Should Be Equal    ${exit_status}    ${my_exit_status}
    Should Be Equal    ${out}            ${my_out}
    Should Be Equal    ${messages}       ${my_messages}

Process Ping Outputs
    [Arguments]
    ...                     ${result}           ${my_result}
    ...                     ${messages}         ${my_messages}

    Log Many                ${result.rc}        ${my_result.rc}
    ...                     ${result.stdout}    ${my_result.stdout}
    ...                     ${result.stderr}    ${my_result.stderr}

    ${out}=                 Trim Ping Output    ${result.stdout}
    ${my_out}=              Trim Ping Output    ${my_result.stdout}

    Compare Ping Outputs    ${result.rc}        ${my_result.rc}
    ...                     ${out}              ${my_out}
    ...                     ${messages}         ${my_messages}

*** Test Cases ***
Test Receiving
    [Documentation]         Basic send and receive 3 times
    [Timeout]               10s

    ${result}               ${messages}=       Test Non Blocking Ping
    ...                     ${PING_BIN}        -c3    ${TEST_ADDRESS}
    ${my_result}            ${my_messages}=    Test Non Blocking Ping
    ...                     ${MY_PING_BIN}     -c3    ${TEST_ADDRESS}

    Process Ping Outputs    ${result}          ${my_result}
    ...                     ${messages}        ${my_messages}

Test Receiving Verbose
    [Documentation]         Basic send and receive 3 times with verbose
    [Timeout]               10s

    ${result}               ${messages}=       Test Non Blocking Ping
    ...                     ${PING_BIN}        -c3    -v    ${TEST_ADDRESS}
    ${my_result}            ${my_messages}=    Test Non Blocking Ping
    ...                     ${MY_PING_BIN}     -c3    -v    ${TEST_ADDRESS}

    Process Ping Outputs    ${result}          ${my_result}
    ...                     ${messages}        ${my_messages}

Test Receiving With Custom Payload
    [Documentation]         Basic send and receive 3 times with a custom payload
    [Timeout]               10s

    ${result}               ${messages}=       Test Non Blocking Ping
    ...                     ${PING_BIN}        -c3    -v    -pcacadebaca    ${TEST_ADDRESS}
    ${my_result}            ${my_messages}=    Test Non Blocking Ping
    ...                     ${MY_PING_BIN}     -c3    -v    -pcacadebaca    ${TEST_ADDRESS}

    Process Ping Outputs    ${result}          ${my_result}
    ...                     ${messages}        ${my_messages}

Test Receiving No Host
    [Documentation]         Basic send and receive 3 times with no host
    [Timeout]               10s

    ${result}               ${messages}=       Test Non Blocking Ping
    ...                     ${PING_BIN}        -c3    -v    count=${0}
    ${my_result}            ${my_messages}=    Test Non Blocking Ping
    ...                     ${MY_PING_BIN}     -c3    -v    count=${0}

    Process Ping Outputs    ${result}          ${my_result}
    ...                     ${messages}        ${my_messages}

Test Receiving Invalid Option
    [Documentation]         Basic send and receive 3 times with invalid option
    [Timeout]               10s

    ${result}               ${messages}=       Test Non Blocking Ping
    ...                     ${PING_BIN}        -c3    -v    -x    count=${0}
    ${my_result}            ${my_messages}=    Test Non Blocking Ping
    ...                     ${MY_PING_BIN}     -c3    -v    -x    count=${0}

    Process Ping Outputs    ${result}          ${my_result}
    ...                     ${messages}        ${my_messages}

Test Receiving Invalid Interval
    [Documentation]         Basic send and receive 3 times with invalid interval
    [Timeout]               10s

    ${result}               ${messages}=       Test Non Blocking Ping
    ...                     ${PING_BIN}        -c3    -v    -i0    count=${0}
    ${my_result}            ${my_messages}=    Test Non Blocking Ping
    ...                     ${MY_PING_BIN}     -c3    -v    -i0    count=${0}

    Process Ping Outputs    ${result}          ${my_result}
    ...                     ${messages}        ${my_messages}

Test Receiving Wrong Host
    [Documentation]         Send and receive 3 time with an unreachable host
    [Timeout]               10s

    ${result}               ${messages}=       Test Blocking Ping
    ...                     ${PING_BIN}        -c3    -v           192.0.2.1    count=${0}
    ${my_result}            ${my_messages}=    Test Blocking Ping
    ...                     ${MY_PING_BIN}     -c3    -v           192.0.2.1    count=${0}

    Process Ping Outputs    ${result}          ${my_result}
    ...                     ${messages}        ${my_messages}

Test Receiving Infinite Setting Count To 0
    [Documentation]         Send and receive infinite times by setting count to 0
    [Timeout]               10s

    ${result}               ${messages}=       Test Blocking Ping
    ...                     ${PING_BIN}        -c0    -v    ${TEST_ADDRESS}
    ${my_result}            ${my_messages}=    Test Blocking Ping
    ...                     ${MY_PING_BIN}     -c0    -v    ${TEST_ADDRESS}

    Process Ping Outputs    ${result}          ${my_result}
    ...                     ${messages}        ${my_messages}

Test Receiving Wrong Payload
    [Documentation]         Send and receive 3 time with a wrong payload
    [Timeout]               10s

    ${result}               ${messages}=       Test Non Blocking Ping
    ...                     ${PING_BIN}        -c3    -v    ${TEST_ADDRESS}    payload=cacadebaca
    ${my_result}            ${my_messages}=    Test Non Blocking Ping
    ...                     ${MY_PING_BIN}     -c3    -v    ${TEST_ADDRESS}    payload=cacadebaca

    Process Ping Outputs    ${result}          ${my_result}
    ...                     ${messages}        ${my_messages}

Test Receiving Wrong Type
    [Documentation]         Send and receive 3 time with a wrong ICMP type
    [Timeout]               10s

    ${result}               ${messages}=       Test Blocking Ping
    ...                     ${PING_BIN}        -c3    -v    ${TEST_ADDRESS}    icmp_type=42
    ${my_result}            ${my_messages}=    Test Blocking Ping
    ...                     ${MY_PING_BIN}     -c3    -v    ${TEST_ADDRESS}    icmp_type=42

    Process Ping Outputs    ${result}          ${my_result}
    ...                     ${messages}        ${my_messages}

Test Receiving Wrong Checksum
    [Documentation]         Send and receive 3 time with a wrong checksum
    [Timeout]               10s

    ${result}               ${messages}=       Test Non Blocking Ping
    ...                     ${PING_BIN}        -c3    -v    ${TEST_ADDRESS}    wrong_checksum=True
    ${my_result}            ${my_messages}=    Test Non Blocking Ping
    ...                     ${MY_PING_BIN}     -c3    -v    ${TEST_ADDRESS}    wrong_checksum=True

    Process Ping Outputs    ${result}          ${my_result}
    ...                     ${messages}        ${my_messages}

Test Receiving Wrong Id
    [Documentation]       Send and receive 3 time with a wrong Id
    [Timeout]             10s

    ${result}               ${messages}=       Test Blocking Ping
    ...                     ${PING_BIN}        -c3    -v    ${TEST_ADDRESS}    wrong_id=True
    ${my_result}            ${my_messages}=    Test Blocking Ping
    ...                     ${MY_PING_BIN}     -c3    -v    ${TEST_ADDRESS}    wrong_id=True

    Process Ping Outputs    ${result}          ${my_result}
    ...                     ${messages}        ${my_messages}

Test Flood
    [Documentation]         Send and receive 1042 messages with flood
    [Timeout]               60s

    ${result}               ${messages}=       Test Non Blocking Ping
    ...                     ${PING_BIN}        -c1042    -v    -f    ${TEST_ADDRESS}    count=1042
    ${my_result}            ${my_messages}=    Test Non Blocking Ping
    ...                     ${MY_PING_BIN}     -c1042    -v    -f    ${TEST_ADDRESS}    count=1042

    Process Ping Outputs    ${result}          ${my_result}
    ...                     ${messages}        ${my_messages}

Test Flood With No Responses
    [Documentation]         Send and receive 42 messages with flood
    [Timeout]               60s

    ${result}               ${messages}=       Test Non Blocking Ping
    ...                     ${PING_BIN}        -c42    -v    -f    ${TEST_ADDRESS}    count=3
    ${my_result}            ${my_messages}=    Test Non Blocking Ping
    ...                     ${MY_PING_BIN}     -c42    -v    -f    ${TEST_ADDRESS}    count=3

    Process Ping Outputs    ${result}          ${my_result}
    ...                     ${messages}        ${my_messages}

Test Wrong Flood Options
    [Documentation]         Test incompatible -i and -f flags
    [Timeout]               10s

    ${result}               ${messages}=       Test Non Blocking Ping
    ...                     ${PING_BIN}        -c1042    -v    -f    -i42
    ...                     ${TEST_ADDRESS}    count=0
    ${my_result}            ${my_messages}=    Test Non Blocking Ping
    ...                     ${MY_PING_BIN}     -c1042    -v    -f    -i42
    ...                     ${TEST_ADDRESS}    count=0

    Process Ping Outputs    ${result}          ${my_result}
    ...                     ${messages}        ${my_messages}
