import os
import subprocess
import pathlib

if __name__ == '__main__':
    glslang_cmd = "glslangValidator"

    shader_list = ["frustum_cul.comp", "g_buffer_fill.vert", "hair.geom", "g_buffer_fill.frag",
                   "g_buffer_resolve.vert", "g_buffer_resolve.geom", "g_buffer_resolve.frag",
                   "landscape.vert", "landscape.frag", "landscape.tese", "landscape.tesc"]

    for shader in shader_list:
        subprocess.run([glslang_cmd, "-V", shader, "-o", "{}.spv".format(shader)])

