#!/usr/bin/env Rscript

#require(rjson)

# aws dynamodb scan --table-name MySimpleEventsTest | jq '.Items[] | select(.DateString.S | test("18/Dec/2020:18:*")) | {"date" : .DateString.S, "value" : .myValue.S }' >test.json
#
#dateExpr="18/Dec/*"
#aws dynamodb query --table-name $tableName --key-conditions '{ "DateString": { "AttributeValueList": [ {"S": "'$dateExpr'"}], "ComparisonOperator": "EQ" }}' | jq --raw-output '.Items | .[] | .MyValue | .S'

#aws dynamodb scan --table-name MySimpleEventsTest | jq -r '.Items[] | select(.DateString.S | test("19/Dec/2020:*")) | [.DateString.S,  .myValue.S ] | @csv ' >test.csv
#aws dynamodb scan --table-name MySimpleEventsTest | jq -r '.Items[] | select(.DateString.S | test("20/Dec/2020:*")) | [.DateString.S,  .myValue.S ] | @csv ' >>test.csv

dat = read.csv("test.csv", header=F, col.names=c("DateString", "Voltage"), stringsAsFactors=F)

#x = as.POSIXlt(dat$DateString[1], format="%d/%b/%Y:%H:%M:%S")
dat$DateString = as.POSIXlt(dat$DateString, format="%d/%b/%Y:%H:%M:%S")
sort_idx = order(dat$DateString)
dat = dat[sort_idx, ]

V_in = 3.3
max_analog = 4095.00;
R1 = 1000000
V_out = dat$Voltage / max_analog * V_in 
#resistance_temp = R1 * ((V_in / V_out) - 1);
#resistance_temp = (V_in - V_out) * R1 / V_out;
resistance_temp = V_in * R1 / V_out - R1

l = log(resistance_temp)
temp = 1/(12.213890099 - 2.494269404*l + 0.170312289*l*l - 0.003883761*l*l*l);


#startTime = as.POSIXlt("18/Dec/2020:18:00:00", format="%d/%b/%Y:%H:%M:%S")
#startTime = as.POSIXlt("19/Dec/2020:00:00:00", format="%d/%b/%Y:%H:%M:%S")
#dat = dat[dat$DateString > startTime, ] 

png("plot.png")
plot(dat$DateString, dat$Voltage, type="l")
dev.off()
