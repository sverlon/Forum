cc_library(
    name = "networking",
    hdrs = [
        "networking.h"
    ]
)

cc_binary(
    name = "forum",
    srcs = ["main.cpp"],
    deps = [
        ":networking"
    ],
    linkopts = [
                   "-lssl",
                   "-lcrypto",
                ],
    data = [ 
        "homepage.html", 
        "style.css",
        "homepage.js"
    ]
)

cc_binary(
    name = "db_server",
    srcs = ["db_server.cpp"],
    deps = [
        ":networking"
    ],
    linkopts = [
                   "-lsqlite3",
                ],
    data = [
        "forum.db"
    ]
)