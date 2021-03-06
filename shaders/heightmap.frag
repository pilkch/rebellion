#version 330

uniform sampler2D texUnit0; // Diffuse texture
uniform sampler2D texUnit1; // Lightmap texture
uniform sampler2D texUnit2; // Detail texture

smooth in vec2 vertOutTexCoord0;
smooth in vec2 vertOutTexCoord1;
smooth in vec2 vertOutTexCoord2;

void main(void)
{
  vec3 diffuse = texture(texUnit0, vertOutTexCoord0).rgb;
  vec3 lightmap = texture(texUnit1, vertOutTexCoord1).rgb;
  vec3 detail = texture(texUnit2, vertOutTexCoord2).rgb;

  gl_FragColor = vec4(diffuse * lightmap * detail, 1.0);
}
