{
    'targets': [
        {
            'target_name': 'fast-ssdeep',
            'sources': [
                'src/ssdeep_node.c',
                'src/polyfill.c',
                
                'ssdeep/fuzzy.c',
                'ssdeep/edit_dist.c'
            ]
        }
    ]
}