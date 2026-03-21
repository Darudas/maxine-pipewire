project('maxine-pipewire', 'c',
    version: '0.1.0',
    license: 'MIT',
    default_options: ['c_std=gnu11', 'warning_level=1'],
)

cc = meson.get_compiler('c')

# ---- Build options ----
opt_video   = get_option('video_effects')
opt_systemd = get_option('systemd')

# ---- Dependencies ----
pipewire_dep = dependency('libpipewire-0.3')
spa_dep      = dependency('libspa-0.2')

dl_dep = dependency('dl', required: false)
if not dl_dep.found()
    dl_dep = cc.find_library('dl')
endif

systemd_dep = dependency('libsystemd', required: true)
threads_dep = dependency('threads', required: opt_video)

inc = include_directories('include')

# ---- Useful paths ----
spa_plugindir = spa_dep.get_variable('plugindir')
prefix        = get_option('prefix')
sysconfdir    = get_option('sysconfdir')
datadir       = get_option('datadir')

# ---- Target 1: libmaxine-core.so ----
core_sources = files(
    'src/maxine_loader.c',
    'src/maxine_effect_registry.c',
    'src/maxine_config.c',
)
core_deps = [dl_dep]

if opt_video
    core_sources += files('src/maxine_vfx_loader.c')
    core_deps += threads_dep
endif

libmaxine_core = shared_library('maxine-core',
    core_sources,
    include_directories: inc,
    dependencies: core_deps,
    install: true,
    version: meson.project_version(),
)

libmaxine_core_dep = declare_dependency(
    link_with: libmaxine_core,
    include_directories: inc,
)

# ---- Target 2: libspa-aec-maxine.so (SPA AEC plugin) ----
shared_library('spa-aec-maxine',
    'src/spa_aec_maxine.c',
    include_directories: inc,
    dependencies: [spa_dep, pipewire_dep, dl_dep, libmaxine_core_dep],
    install: true,
    install_dir: spa_plugindir / 'aec',
    name_prefix: 'lib',
)

# ---- Target 3: maxined (daemon) ----
executable('maxined',
    files(
        'src/maxined.c',
        'src/maxine_audio_node.c',
        'src/maxine_audio_chain.c',
        'src/maxine_dbus.c',
    ),
    include_directories: inc,
    dependencies: [pipewire_dep, spa_dep, systemd_dep, libmaxine_core_dep],
    install: true,
)

# ---- Target 4: maxctl (CLI tool) ----
executable('maxctl',
    'src/maxctl.c',
    include_directories: inc,
    dependencies: [systemd_dep],
    install: true,
)

# ---- Install: config template ----
install_data(
    'config/60-maxine-echo-cancel.conf',
    install_dir: sysconfdir / 'maxine-pipewire',
)

# ---- Configure data for template substitution ----
conf_data = configuration_data()
conf_data.set('bindir', prefix / get_option('bindir'))

# ---- Install: systemd user service ----
if opt_systemd
    systemd_user_unit_dir = systemd_dep.get_variable(
        pkgconfig: 'systemduserunitdir',
        default_value: prefix / 'lib' / 'systemd' / 'user',
    )

    configure_file(
        input: 'config/maxined.service.in',
        output: 'maxined.service',
        configuration: conf_data,
        install: true,
        install_dir: systemd_user_unit_dir,
    )
endif

# ---- Install: D-Bus service activation file ----
dbus_services_dir = datadir / 'dbus-1' / 'services'

configure_file(
    input: 'config/com.github.maxine_pipewire.Daemon.service.in',
    output: 'com.github.maxine_pipewire.Daemon.service',
    configuration: conf_data,
    install: true,
    install_dir: dbus_services_dir,
)

# ---- Summary ----
summary({
    'prefix':         prefix,
    'SPA plugin dir': spa_plugindir / 'aec',
    'video_effects':  opt_video,
    'systemd':        opt_systemd,
}, section: 'Configuration')
