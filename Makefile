run:
	./build_and_run.sh

clean:
	rm -rf apex/build
	rm -rf apex/cmake-build-debug
	rm -rf build
	rm -rf cmake-build-debug
	rm -rf dg/build
	@find . -name '__pycache__' -exec rm -fr {} +
	@find . -name '*.pyc' -exec rm -f {} +
	@find . -name '*.pyo' -exec rm -f {} +
	@find . -name '*.un~' -exec rm -f {} +
