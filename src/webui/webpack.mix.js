let mix = require('laravel-mix');
require('laravel-mix-copy-watched');
require('laravel-mix-purgecss');

/*
 |--------------------------------------------------------------------------
 | Mix Asset Management
 |--------------------------------------------------------------------------
 |
 | Mix provides a clean, fluent API for defining some Webpack build steps
 | for your Laravel application. By default, we are compiling the Sass
 | file for your application, as well as bundling up your JS files.
 |
 */

mix.js([
	'src/init.js', 
	'src/websockets/ws_rtaudio.js',
	'src/app.js', 
	'src/notify.js', 
	'src/websockets/ws_baresip.js',
	'src/websockets/ws_calls.js',
	'src/websockets/ws_meter.js',
	'src/websockets/ws_contacts.js',
	'src/websockets/ws_options.js',
	'src/onboarding.js'
], 'dist/app.js');

mix.sass('src/app.scss', 'dist/')
   .purgeCss({
		enabled: mix.inProduction(),
		folders: ['src', 'node_modules/bootbox/dist', 'node_modules/shepherd.js'],
		extensions: ['html', 'js', 'handlebars', 'vue'],
	   	whitelistPatterns: [/shepherd-.*/],
   })
   .copyWatched('src/index.html', 'dist/')
   .copyDirectory('src/images', 'dist/images');

mix.webpackConfig({
	module: {
		rules: [
			{
				test: /\.handlebars?$/,
				loader: 'handlebars-loader'
			}
		]
	}
});
