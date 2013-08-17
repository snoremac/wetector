export WETECTOR_HOME = .
export WETECTOR_SRC = $(WETECTOR_HOME)/src/wetector
export WETECTOR_BUILD = $(WETECTOR_HOME)/build
export SENSIMATIC_HOME = $(WETECTOR_HOME)/../sensimatic
export SENSIMATIC_SRC = $(SENSIMATIC_HOME)/src/sensimatic
export BUILD_DIR = $(WETECTOR_BUILD)

.PHONY: all debug install
all : wetector
debug : wetector-debug
install : wetector-install

include $(SENSIMATIC_HOME)/mk/defs.mk
include $(SENSIMATIC_HOME)/mk/common.mk

DEFAULT_LDFLAGS += -lprintf_flt -Wl,-u,vfprintf

INCLUDES = -I. -I$(WETECTOR_SRC) -I$(WETECTOR_BUILD) -I$(SENSIMATIC_SRC)

.PHONY: wetector
wetector : $(WETECTOR_HOME)/wetector.hex

wetector_src = $(wildcard $(WETECTOR_SRC)/*.c)
wetector_obj = $(WETECTOR_BUILD)/wetector/pgm_strings.o $(wetector_src:.c=.o)

# Empty SECONDARY stops intermediary files (pgm_strings.c)
# from being deleted by make
.SECONDARY:

$(WETECTOR_BUILD)/wetector/pgm_strings.o : $(WETECTOR_BUILD)/wetector/pgm_strings.h

$(WETECTOR_BUILD)/wetector/pgm_strings.% : res/pgm_strings.%.erb
	mkdir -p $(WETECTOR_BUILD)/wetector
	erb -r yaml $< > $@

$(WETECTOR_HOME)/wetector.elf : $(wetector_obj) $(SENSIMATIC_SRC)/sensimatic.a
	$(CC) $(DEFAULT_LDFLAGS) $(LDFLAGS) -o $@ $^

.PHONY: $(SENSIMATIC_SRC)/sensimatic.a
$(SENSIMATIC_SRC)/sensimatic.a : 
	make -C $(SENSIMATIC_HOME) sensimatic INCLUDES=-I$(realpath $(WETECTOR_SRC)) LOG_LEVEL=$(LOG_LEVEL)

.PHONY: clean
clean:
	rm -rf $(WETECTOR_BUILD)
	rm -f $(addprefix $(WETECTOR_HOME)/, $(ARTIFACTS))
	rm -f $(addprefix $(WETECTOR_SRC)/, $(ARTIFACTS))
	rm -f $(addprefix $(SENSIMATIC_SRC)/, $(ARTIFACTS))
	make -C $(SENSIMATIC_HOME) clean
