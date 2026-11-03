# dmgbuild settings: input paths are supplied by create-dmg.sh.
files = [defines['app']]
symlinks = {'Applications': '/Applications'}
background = defines['background']
icon_locations = {'Lucid.app': (220, 240), 'Applications': (540, 240)}
window_rect = ((100, 100), (760, 450))
icon_size = 112
text_size = 14
show_status_bar = False
show_toolbar = False
show_pathbar = False
show_sidebar = False
format = 'UDZO'
