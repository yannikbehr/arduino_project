# Define the environment if you have multiple in platformio.ini, 
# otherwise leave empty to build/upload all.
ENV = 

.PHONY: build upload clean monitor monitor-log

# Build the project
build:
	pio run $(if $(ENV),-e $(ENV))

# Build and upload
upload:
	pio run $(if $(ENV),-e $(ENV)) -t upload

# Clean the build files
clean:
	pio run $(if $(ENV),-e $(ENV)) -t clean

# Open the serial monitor (interactive)
monitor:
	pio device monitor

# Non-interactive capture — writes to log.txt, usable by Claude Code.
# Usage: make monitor-log          (90s default)
#        make monitor-log DURATION=120
DURATION ?= 90
monitor-log:
	python3 monitor_log.py $(DURATION) | tee log.txt


certs/device.crt:
	@mkdir -p certs
	@echo Extract the certificate
	cd terraform && terraform output -raw certificate_pem > ../certs/device.crt

certs/device.key:
	@mkdir -p certs
	@echo "Extract the private key (you might need to use 'terraform console' if it's marked sensitive)"
	cd terraform && terraform output -raw private_key > ../certs/device.key

certs/AmazonRootCA1.pem:
	@mkdir -p certs
	@echo Download the Amazon Root CA
	curl https://www.amazontrust.com/repository/AmazonRootCA1.pem -o certs/AmazonRootCA1.pem

certs: certs/device.crt certs/device.key certs/AmazonRootCA1.pem
