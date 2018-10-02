if [ ! -e config.mk ]; then
    echo "run ./configure or ./configure-mingw first." >&2
    exit 1
fi
