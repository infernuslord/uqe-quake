const char *passf = STRINGIFY(
uniform sampler2D source;

void main(void)
{
	vec4 color = texture2D(source, gl_TexCoord[0].st);

	gl_FragColor = color;
}
);