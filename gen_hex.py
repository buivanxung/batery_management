Import("env")

env.AddPostAction(
    "$BUILD_DIR/${PROGNAME}.elf",
    env.VerboseAction(
        " ".join([
            "$OBJCOPY",
            "-O",
            "ihex",
            "$BUILD_DIR/${PROGNAME}.elf",
            "$BUILD_DIR/${PROGNAME}.hex"
        ]),
        "Building HEX file"
    )
)