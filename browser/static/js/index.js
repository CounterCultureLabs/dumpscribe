

var router = new Grapnel();


function init() {

    router.get('', function(req){
        $.getJSON('notebooks', function(val) {
            var notebooks = val.data;
            var tmpl = _.template($('#notebook-summary-template').html());
            
            var html = '';
            var key, notebook;
            for(key in notebooks) {
                notebook = notebooks[key];
                html += tmpl(notebook);
            }
            $('#container').html(html);
            
            $('.notebook-summary').click(function(e) {
                var nbs = $(e.target).closest('.notebook-summary');
                var id = nbs.find('.id').val();
                window.location = '#notebook/' + id;
            });
        });
    });
    
    router.get('notebook/*', function(req) {
        
        $.getJSON('notebook/'+req.params[0], function(val) {
            var pages = val.data;
            var tmpl = _.template($('#page-template').html());
            var html = '';
            var i;
            for(i=0; i < pages.length; i++) {
                html += tmpl(pages[i]);
            }
            $('#container').html(html);
        });
        
    });
}


$(document).ready(init);
