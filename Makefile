# Forward public build goals without duplicating engine build rules.
.DEFAULT_GOAL := all
GOALS := $(if $(MAKECMDGOALS),$(MAKECMDGOALS),all)
.PHONY: forward $(GOALS)
$(GOALS): forward
forward:
	$(MAKE) -C fuselage $(GOALS)
