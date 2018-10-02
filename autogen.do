exec >&2
(find wvstreams -name '*.in' -o -name '*.ac' | xargs redo-ifchange) &&
redo-ifchange wvstreams/autogen.sh &&
cd wvstreams &&
./autogen.sh
