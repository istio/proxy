var Stream = require( 'stream' )

/**
 * Browserify transform that replaces an unnamed
 * AMD define with a named define
 * @param  {Object} browserify
 * @param  {Object} options
 * @return {Object} browserify
 */
module.exports = function namedAMD( browserify, options ) {

  options = options != null ? options : {}

  var transform = new Stream.Transform({
    objectMode: true,
    transform: function( chunk, encoding, next ) {

      if( !this._processed && typeof options.name === 'string' ) {
        var definition = chunk.toString().replace(
          '&&define.amd){define([],f)',
          '&&define.amd){define(\'' + options.name + '\',[],f)'
        )
        this._processed = true
        return next( null, new Buffer( definition ) )
      }

      next( null, chunk )

    }
  })

  // Append transform to internal pipeline's last step
  browserify.pipeline.get( 'wrap' )
    .push( transform )

  return browserify

}
