# Use the official AWS Lambda Python 3.10 base image
FROM public.ecr.aws/lambda/python:3.10

# Install Python packages
RUN pip install --no-cache-dir plotly pandas

# Copy the function code into the container
COPY app.py ${LAMBDA_TASK_ROOT}

# Set the CMD to your handler (filename.handler_function)
CMD ["app.lambda_handler"]
