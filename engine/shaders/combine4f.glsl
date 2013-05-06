const char *combine4f = STRINGIFY(
uniform sampler2D Pass0;
uniform sampler2D Pass1;
uniform sampler2D Pass2;
uniform sampler2D Scene;

const float xBaseSaturation = 1.0;
const float xBaseIntensity = 0.8;

const float xBloomSaturation = 1.0;
const float xBloomIntensity = 0.215;

vec4 AdjustSaturation(vec4 Color, float Saturation)
{
	// the constants 0.3, 0.59 and 0.11 are chosen because the human eye is more sensitive to green light and less to blue light.
	float Gray = dot(Color, vec4(0.3, 0.59, 0.11, 1.0));
	
	return mix(vec4(Gray), Color, Saturation); // lerp(Gray, Color, Saturation); // lerp had issues on ATI Radeon HD5770
}

void main(void)
{
	// get the original base texture pixel color as well as the bloom pixel color
    vec4 t0 = texture2D(Pass0, gl_TexCoord[0].st);
    vec4 t1 = texture2D(Pass1, gl_TexCoord[0].st);
    vec4 t2 = texture2D(Pass2, gl_TexCoord[0].st);

	vec4 bloom = t0 + t1 + t2;
	vec4 base = texture2D(Scene, gl_TexCoord[0].st);


	// adjust pixel color saturation and intensity
	base = AdjustSaturation(base, xBaseSaturation) * xBaseIntensity;
	bloom = AdjustSaturation(bloom, xBloomSaturation) * xBloomIntensity;


	// darken the base image in areas where there is a lot of bloom
	// to prevent things looking excessively burned-out.
	base *= (1 - clamp(bloom, 0.0, 1.0));


	gl_FragColor = base + bloom;
}
);