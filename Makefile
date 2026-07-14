# ZDoc — top-level build.
#
# Each component under parser/, extractor/, ai/, renderer/ and the zdoc/ CLI
# builds its own standalone `zdoc-<module>` executable from its own Makefile
# (see docs/ZDOC.md -> Architecture). This root Makefile just fans `make` out
# into every subdirectory that has a Makefile, so the whole project builds,
# tests and cleans with a single command. Components that don't have a
# Makefile yet are skipped, so the build stays green while the tree is still
# being filled in.

# Component directories (one or two levels deep) that ship a Makefile.
MODULES := $(patsubst %/Makefile,%,$(wildcard */Makefile */*/Makefile))

.PHONY: all test clean list

all clean:
	@if [ -z "$(strip $(MODULES))" ]; then \
		echo "zdoc: no component Makefiles yet — nothing to $@ (see docs/ZDOC.md)"; \
	else \
		for d in $(MODULES); do \
			echo "==> $@ $$d"; \
			$(MAKE) -C $$d $@ || exit $$?; \
		done; \
	fi

# Unlike all/clean, keep going when a component's tests fail so every
# component still gets tested, then exit non-zero if any of them failed.
test:
	@if [ -z "$(strip $(MODULES))" ]; then \
		echo "zdoc: no component Makefiles yet — nothing to $@ (see docs/ZDOC.md)"; \
	else \
		fail=0; \
		for d in $(MODULES); do \
			echo "==> $@ $$d"; \
			$(MAKE) -C $$d $@ || fail=1; \
		done; \
		exit $$fail; \
	fi

# Show which components are currently wired into the build.
list:
	@if [ -z "$(strip $(MODULES))" ]; then \
		echo "(no component Makefiles yet)"; \
	else \
		for d in $(MODULES); do echo "  $$d"; done; \
	fi
