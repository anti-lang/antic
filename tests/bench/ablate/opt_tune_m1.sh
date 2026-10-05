#!/bin/sh
# Ablation wrapper: writes "tune-cpu"="apple-m1" after
# "target-cpu"="generic" in the text and passes the result on. The
# instructions of the CPU level stay, and the scheduling model changes.
#
# antic runs a wrapper as `--opt`, with the options of opt and the path of
# the LLVM text last. The pinned opt is bin/opt of the runtime directory in
# ANTI_ABLATE_RUNTIME. ANTI_ABLATE_NEXT lists further wrappers, separated
# by colons, that run before it. run.py sets both. The edited text goes to
# the next program on its standard input, so no file is left behind.
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
for text; do :; done
n=$#
i=0
for a; do
	i=$((i + 1))
	if [ $i -lt $n ]; then
		set -- "$@" "$a"
	fi
done
shift $n
tune='s/"target-cpu"="generic"/"target-cpu"="generic" "tune-cpu"="apple-m1"/g'
if [ "$text" = - ]; then
	sed "$tune"
else
	sed "$tune" "$text"
fi | "$next" "$@" -
