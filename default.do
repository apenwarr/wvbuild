exec >&2
case "$1" in
    wvstreams/*)
    	redo-ifchange config.mk wvconfig
        target=${1#wvstreams/}
        . ./config.mk
        redo-always &&
        exec make -C "$MYHOST" "$target"
        ;;
    *)
        echo "Don't know how to make '$1'" >&2
        exit 1
        ;;
esac