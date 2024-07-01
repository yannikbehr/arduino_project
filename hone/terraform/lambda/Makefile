

ECR=137637932857.dkr.ecr.eu-west-3.amazonaws.com
repo=hone-lambda-ecr
#version=1
region=eu-west-3

version_old=$(shell aws ecr list-images --repository-name hone-lambda-ecr | jq -r '.["imageIds"][] | .["imageTag"]' | sort -n | tail -n 1)
version=$(shell python -c "print($(version_old)+1)")

echo_version:
	@echo Next version is: ${version}

push:
	docker build --platform linux/amd64 -t lambda-fun .
	docker tag lambda-fun:latest ${ECR}/${repo}:${version}
	aws ecr get-login-password --region ${region}  | docker login --username AWS --password-stdin ${ECR}
	docker push ${ECR}/${repo}:${version}
