exec >&2
redo-ifchange patch &&
rm -f build/w32api &&
ln -s . build/w32api
