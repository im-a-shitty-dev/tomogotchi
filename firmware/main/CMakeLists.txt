# No REQUIRES list: main depends on every component, including the managed
# ones from idf_component.yml and our own components/ (pet).
idf_component_register(
    SRCS "main.c" "screen.c" "motion.c" "net.c" "rtc.c"
    INCLUDE_DIRS "."
)
