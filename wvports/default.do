exec >&2

contains() {
    [ "$1" != "${1#*$2}" ]
}

path=$2
cmd=${path##*/}
port=${path%/"$cmd"}

do_extract() (
    P=$port
    [ -n "$BUILDDIR" ] || BUILDDIR=$P/build

    rm -rf "$BUILDDIR" &&
    mkdir -p "$BUILDDIR" &&
    (cd "$BUILDDIR" &&
     for d in ../sources/*.rpm ../sources/*.tar.bz2 ../sources/*.tar.Z ../sources/*.tgz ../sources/*.tar.gz ../sources/*.zip; do
            [ -e "$d" ] || continue
            echo "*** Unpacking $d..."
            redo-ifchange "$d"
            case $d in
              *.rpm)
                rpm2cpio "$d" | cpio -id
                ;;
              *.tar.bz2)
                tar jxf "$d"
                ;;
              *.tar.Z|*.tgz|*.tar.gz)
                tar zxf "$d"
                ;;
              *.zip)
                unzip "$d"
                ;;
              *)
                echo "Unknown file type: $d" >&2
                exit 1
            esac || exit 1
     done
    ) || exit 1
    (cd "$BUILDDIR" &&
     for d in *; do
       [ ! -d "$P" ] || break
       if [ -d "$d" ] && [ ! -L "$d" ]; then
         ln -s "$d" "$P"
       fi
     done
    ) || exit 1
    
    # FIXME(apenwarr): redo should do the right thing with a static stampfile.
    #  Right now, if $P/patch.do calls 'redo $P/extract.do', then the static
    #  stamp file will get replaced and redo won't notice until the next run,
    #  causing a spurious re-extraction next time. Using a different filename
    #  each time dodges the problem, but it's a bit silly.
    STAMP=$BUILDDIR/.wvports.extracted.$(perl -e 'print time().".$$.".rand()')
    touch "$STAMP"
    redo-ifchange "$STAMP"
)

do_patch() (
    P=$port
    [ -n "$BUILDDIR" ] || BUILDDIR="$P/build"

    if [ -e "$BUILDDIR/.wvports.patch" ]; then
        echo "$P/patch: already patched; need to re-extract." >&2
        redo "$P/extract"
    fi

    redo-ifchange "$P/extract"

    PATCHES=$(echo $P/patches/*/*.diff $P/patches/*/*.patch)

    # mark the directory as "at least partially patched"
    touch "$BUILDDIR/.wvports.patch"

    for dir in $P/patches/*; do
        [ -d "$dir" ] || continue
        basedir=${dir##*/}
        for d in $dir/*.diff $dir/*.patch; do
            [ -e "$d" ] || continue
            echo "*** Applying patch $d..."
            redo-ifchange "$d"
            patch -d "$BUILDDIR/$basedir" -N -p1 <$d || exit 1
        done
    done
)

if contains "$path" "/" && ! contains "$port" "/" && [ -d "$port" ]; then
    case $cmd in
      extract)
        redo-ifchange ../config.mk &&
        do_extract
        ;;
      patch)
        redo-ifchange "$port/extract" &&
        do_patch
        ;;
      prepare)
        redo-ifchange "$port/patch" &&
        echo "no $port/$cmd.do, skipping step."
        ;;
      compile)
        redo-ifchange "$port/prepare" &&
        echo "no $port/$cmd.do, skipping step."
        ;;
      all)
        redo-ifchange "$port/compile"
        ;;
      clean)
        rm -rf "$port/build"
        ;;
      *)
        echo "Unknown wvports step: $cmd" >&2
        exit 1
        ;;
    esac
    exit $?
fi

echo "Unknown target: $1"
exit 1
