if [ ! -e _config.mk ]; then
    echo "run ./configure or ./configure-mingw first." >&2
    exit 1
fi
redo-ifchange _config.mk
cat _config.mk >$3
redo-stamp <$3
