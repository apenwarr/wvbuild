exec >&2
redo-ifchange ../config.mk ../config2.mk &&
(find $2/sources -type f | xargs redo-ifchange) &&
([ ! -d $2/patches ] || find $2/patches -type f | xargs redo-ifchange) &&
make -C $2 &&
(find $2/build -type f | xargs redo-ifchange)
