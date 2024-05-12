#!/bin/bash
#
# author: Jonas Behr 
# date: Sun Mar  3 16:17:43 2024
######################################

# Install dependencies
pip3 install -r requirements.txt -t ./ 

# Zip the code directory
zip -r lambda_function.zip lambda_function.py pytz 

