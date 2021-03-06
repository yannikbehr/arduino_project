

aws dynamodb scan --table-name MySimpleEventsTest | jq '.Items[] | select(.DateString.S | test("18/Dec/2020:1*")) | .myValue.S ' 
