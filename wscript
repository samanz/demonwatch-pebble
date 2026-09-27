import os.path

top = '.'
out = 'build'

def options(ctx):
    ctx.load('pebble_sdk')

def configure(ctx):
    ctx.load('pebble_sdk')

def build(ctx):
    ctx.load('pebble_sdk')

    binaries = []
    cached_env = ctx.env

    cflags = [
        '-w',
        '-Wno-error',
        '-flto',
        '-fomit-frame-pointer',
        '-fno-stack-protector',
        '-fno-exceptions',
        '-fno-unwind-tables',
        '-fno-asynchronous-unwind-tables',
        '-DFLAT_SPAN',
        '-DFLAT_NUKAGE1_COLOR=118',
        '-DVIEWWINDOWWIDTH=120',
        '-DVIEWWINDOWHEIGHT=114',
        '-DMAPWIDTH=120',
        '-DLOW_MEMORY',
        '-DPEBBLE_EMERY',
        '-DC_ONLY=1',
    ]

    includes = [
        ctx.path.abspath(),
        ctx.path.find_dir('src').abspath(),
        ctx.path.find_dir('src/doom').abspath(),
        ctx.path.find_dir('src/pebble').abspath(),
    ]

    for platform in ctx.env.TARGET_PLATFORMS:
        ctx.env = ctx.all_envs[platform]
        ctx.env.append_value('CFLAGS', cflags)
        ctx.env.append_value('LINKFLAGS', ['-flto', '-Os', '-Wl,-u,__pbl_app_info'])
        platform_includes = includes + [
            ctx.path.find_or_declare('build/' + platform).abspath(),
            ctx.path.find_or_declare('build/' + platform + '/src').abspath(),
        ]
        ctx.env.append_value('INCLUDES', platform_includes)

        ctx.set_group(ctx.env.PLATFORM_NAME)
        app_elf = '{}/pebble-app.elf'.format(ctx.env.BUILD_DIR)
        
        # Collect all C files in src/doom and src/pebble
        c_sources = ctx.path.ant_glob(['src/doom/*.c', 'src/pebble/*.c'])
        ctx.pbl_build(source=c_sources, target=app_elf, bin_type='app')
        binaries.append({'platform': platform, 'app_elf': app_elf})

    ctx.env = cached_env
    ctx.set_group('bundle')
    ctx.pbl_bundle(binaries=binaries)
