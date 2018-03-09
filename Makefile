build:
	bash testpass/build_and_run.sh

clean:
	rm -rf testpass/build
	rm -rf cmake-build-debug
	@find . -name '__pycache__' -exec rm -fr {} +
	@find . -name '*.pyc' -exec rm -f {} +
	@find . -name '*.pyo' -exec rm -f {} +
	@find . -name '*.un~' -exec rm -f {} +
