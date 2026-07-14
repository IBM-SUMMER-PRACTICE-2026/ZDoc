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

.PHONY: all test clean list dist

all test clean:
	@if [ -z "$(strip $(MODULES))" ]; then \
		echo "zdoc: no component Makefiles yet — nothing to $@ (see docs/ZDOC.md)"; \
	else \
		for d in $(MODULES); do \
			echo "==> $@ $$d"; \
			$(MAKE) -C $$d $@ || exit $$?; \
		done; \
	fi

# Build everything, then collect the release artifacts into ./dist.
dist: all
	@sh scripts/collect-artifacts.sh

# Show which components are currently wired into the build.
list:
	@if [ -z "$(strip $(MODULES))" ]; then \
		echo "(no component Makefiles yet)"; \
	else \
		for d in $(MODULES); do echo "  $$d"; done; \
	fi
