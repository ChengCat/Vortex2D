#version 150

precision highp float;

in vec2 a_Position;
in vec2 a_TexCoords;

uniform mat4 u_Projection;
uniform vec2 h;

out vec2 v_texCoord;
out vec2 v_texCoordxp;
out vec2 v_texCoordxn;
out vec2 v_texCoordyp;
out vec2 v_texCoordyn;

void main()
{
    gl_Position = u_Projection * vec4(a_Position, 0.0, 1.0);
	v_texCoord = a_TexCoords;
    
    vec2 dx = vec2(1.0/h.x, 0.);
    vec2 dy = vec2(0., 1.0/h.y);
    
	v_texCoordxp = v_texCoord + dx; 
	v_texCoordxn = v_texCoord - dx; 
	v_texCoordyp = v_texCoord + dy;
	v_texCoordyn = v_texCoord - dy;
}