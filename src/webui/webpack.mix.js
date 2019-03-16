let mix = require('laravel-mix');
require('laravel-mix-copy-watched');

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
	'src/app.js', 
	'src/websockets/test.js'
], 'dist/app.js');

mix.sass('src/app.scss', 'dist/')
   .copyWatched('src/index.html', 'dist/')
   .copyDirectory('src/images', 'dist/images')
   .copy('node_modules/@fortawesome/fontawesome-free/webfonts', 'dist/fonts');


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
