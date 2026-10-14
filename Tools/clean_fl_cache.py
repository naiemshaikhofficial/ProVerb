import os

p = r'C:\Users\Naiem Shaikh\Documents\Image-Line\FL Studio\Presets\Plugin database\Installed\VerifiedIDs.nfo'
if os.path.exists(p):
    os.system(f'attrib -H -R "{p}"')
    with open(p, 'r', encoding='utf-8', errors='ignore') as fp:
        lines = fp.readlines()
    new_lines = [l for l in lines if 'ProVerb' not in l]
    with open(p, 'w', encoding='utf-8') as fp:
        fp.writelines(new_lines)
    os.system(f'attrib +H "{p}"')
    print('Cleaned VerifiedIDs.nfo!')

fl_ini = r'C:\Users\Naiem Shaikh\Documents\Image-Line\FL Studio\Presets\Plugin database\Installed\.Plugins.ini'
if os.path.exists(fl_ini):
    os.system(f'attrib -H -R "{fl_ini}"')
    with open(fl_ini, 'r', encoding='utf-8', errors='ignore') as fp:
        lines = fp.readlines()
    new_lines = [l for l in lines if 'ProVerb' not in l]
    with open(fl_ini, 'w', encoding='utf-8') as fp:
        fp.writelines(new_lines)
    os.system(f'attrib +H "{fl_ini}"')
    print('Cleaned .Plugins.ini!')
