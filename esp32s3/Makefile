# Define the environment if you have multiple in platformio.ini, 
# otherwise leave empty to build/upload all.
ENV = 

.PHONY: build upload clean monitor

# Build the project
build:
	pio run $(if $(ENV),-e $(ENV))

# Build and upload
upload:
	pio run $(if $(ENV),-e $(ENV)) -t upload

# Clean the build files
clean:
	pio run $(if $(ENV),-e $(ENV)) -t clean

# Open the serial monitor
monitor:
	pio device monitor
