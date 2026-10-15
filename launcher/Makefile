# Custom launcher app for MP5 X7 / ATJ2273S (Actions AOS / USDK).
# Build with the SDK rules; SDKROOT must point to the "sdk" tree (see build.sh).
#
#   make SDKROOT=/path/to/sdk
#
IMAGENAME = launcher

# SDK root (absolute path, passed by build.sh)
ROOT      = $(SDKROOT)

# Load/virtual address for this app (from sdk/case/include/case.xn)
VADDR     = $(LAUNCHER_VADDR)

# where the .app/.lst/.map go
IMGDIR    = ./out

include $(ROOT)/case/makefile/makefile_app.rules
