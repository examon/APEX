build:
	cd src; \
	./build.sh;

doc:
	cd src; \
	doxygen Doxyfile

clean:
	rm -rf build
	rm -rf cmake-build-debug
	rm -rf src/apex.log
	rm -rf src/apex.out
	rm -rf src/apex/build
	rm -rf src/apex/cmake-build-debug
	rm -rf src/build
	rm -rf src/cmake-build-debug
	rm -rf src/dg/build
	@find . -name '__pycache__' -exec rm -fr {} +
	@find . -name '*.pyc' -exec rm -f {} +
	@find . -name '*.pyo' -exec rm -f {} +
	@find . -name '*.un~' -exec rm -f {} +
