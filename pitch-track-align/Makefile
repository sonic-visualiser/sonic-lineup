
SCRIPTS	:= ../../sml-buildscripts

pitch-track-align:	pitch-track-align.mlb pitch-track-align.deps
	mlton pitch-track-align.mlb

pitch-track-align.deps: pitch-track-align.mlb 
	${SCRIPTS}/mlb-dependencies $^ > $@

clean:
	rm -f pitch-track-align *.deps

-include *.deps
