all: build

build:
	@$(MAKE) -C "src" --no-print-directory $(MAKEARGS)

clean:
	@rm -rvf fastdup src/*.o modules/*.so
