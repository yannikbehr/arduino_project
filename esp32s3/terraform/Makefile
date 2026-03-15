
invoke_lambda:
	aws lambda invoke --function-name esp32_sensor_processor --payload '{"device_id": "SIM7080G_01", "temp": 24.5, "volt": 3.9}' ret.json
	@echo "output from lambda"
	@echo $(shell cat ret.json)

describe_endpoint: 
	aws iot describe-endpoint --endpoint-type iot:Data-ATS

send_data: 
	aws iot-data publish --topic "devices/SIM7080G_01/data" --payload '{"temp": 25.1111, "status": "active"}' --endpoint-url "https://a3fu7j5avgf87g-ats.iot.eu-west-3.amazonaws.com"


