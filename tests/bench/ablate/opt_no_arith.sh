#!/bin/sh
# Ablation wrapper: removes the facts of arithmetic, addresses and ranges,
# nsw and nuw, inbounds on a getelementptr, !range on a load and range() on
# a result and a call, and the calls and declarations of the lifetime
# intrinsics, from the text and passes the result on. It measures a program
# as it was before the step arith.
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
facts='s/ (nuw|nsw)//g
s/getelementptr inbounds /getelementptr /g
s/, !range ![0-9]+//g
s/range\(i[0-9]+ -?[0-9]+, -?[0-9]+\) //g
/@llvm\.lifetime\.(start|end)\.p0\(/d'
if [ "$text" = - ]; then
	sed -E "$facts"
else
	sed -E "$facts" "$text"
fi | "$next" "$@" -
