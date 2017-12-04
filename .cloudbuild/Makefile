#PROJECT ?= istio-testing
VERSION ?= 0.4.4
#HUB ?= gcr.io/

PROJECT ?= costinm
HUB ?=

# Note: The build directory is the root of the istio/test-infra repository, not ./
image:
	docker build -t ${HUB}$(PROJECT)/proxy-gcb-builder:$(VERSION) -f Dockerfile .


push:
	gcloud docker -- push "${HUB}$(PROJECT)/proxy-gcb-builder:$(VERSION)"

latest:
	docker tag ${HUB}$(PROJECT)/proxy-gcb-builder:$(VERSION) ${HUB}$(PROJECT)/proxy-builder:latest
	gcloud docker -- push "${HUB}$(PROJECT)/proxy-gcb-builder:latest"


.PHONY: image push latest
