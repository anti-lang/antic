#!/bin/sh
# Ablation wrapper: removes !tbaa from every load and store of the text and
# passes the result on. The nodes of the tree stay, and no access names
# them. It measures a program as it was before the step tbaa.
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
facts='s/, !tbaa ![0-9]+//g'
if [ "$text" = - ]; then
	sed -E "$facts"
else
	sed -E "$facts" "$text"
fi | "$next" "$@" -
