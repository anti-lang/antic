#!/bin/sh
# Ablation wrapper: runs the pipeline default<O3> in place of the one
# antic passes, and passes every other option on.
#
# antic runs a wrapper as `--opt`, with the options of opt and the path of
# the LLVM text last. The pinned opt is bin/opt of the runtime directory in
# ANTI_ABLATE_RUNTIME. ANTI_ABLATE_NEXT lists further wrappers, separated
# by colons, that run before it. run.py sets both.
if [ -n "$ANTI_ABLATE_NEXT" ]; then
	next=${ANTI_ABLATE_NEXT%%:*}
	case $ANTI_ABLATE_NEXT in
	*:*) ANTI_ABLATE_NEXT=${ANTI_ABLATE_NEXT#*:} ;;
	*) ANTI_ABLATE_NEXT= ;;
	esac
	export ANTI_ABLATE_NEXT
else
	next=$ANTI_ABLATE_RUNTIME/bin/opt
fi
n=$#
for a; do
	case $a in
	-passes=*) set -- "$@" '-passes=default<O3>' ;;
	*) set -- "$@" "$a" ;;
	esac
done
shift $n
exec "$next" "$@"
