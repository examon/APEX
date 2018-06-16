clean:
	rm -rf testpass/build
	rm -rf testpass/cmake-build-debug
	rm -rf build
	rm -rf cmake-build-debug
	@find . -name '__pycache__' -exec rm -fr {} +
	@find . -name '*.pyc' -exec rm -f {} +
	@find . -name '*.pyo' -exec rm -f {} +
	@find . -name '*.un~' -exec rm -f {} +
