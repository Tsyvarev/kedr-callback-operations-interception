<?xml version="1.0" encoding="utf-8"?>
<!DOCTYPE article PUBLIC "-//OASIS//DTD DocBook XML V4.5//EN"
    "@DOCBOOK_DTD_FILE@" [ 
        <!ENTITY % kedr-coi-entities SYSTEM "entities.xml"> %kedr-coi-entities; 
        <!ENTITY kedr_coi_intro SYSTEM "intro.xml">
        <!ENTITY kedr_coi_overview SYSTEM "overview.xml">
        <!ENTITY kedr_coi_install SYSTEM "install.xml">
        <!ENTITY kedr_coi_using SYSTEM "using_kedr_coi.xml">
        <!ENTITY kedr_coi_description SYSTEM "description.xml">
        <!ENTITY objects_watching SYSTEM "objects_watching.xml">
        <!ENTITY kedr_coi_payload SYSTEM "payload.xml">
        <!ENTITY kedr_coi_interceptor_creation SYSTEM "interceptor_creation.xml">
        <!ENTITY kedr_coi_foreign_interceptor_creation SYSTEM "foreign_interceptor_creation.xml">
        <!ENTITY pre_existed_interceptors SYSTEM "pre_existed_interceptors.xml">
        <!ENTITY create_interceptors_using_kedr_gen SYSTEM "create_interceptors_using_kedr_gen.xml">
        <!ENTITY api_reference SYSTEM "api_reference.xml">
        
        <!ENTITY kedr_coi_glossary SYSTEM "glossary.xml">
    ]
>

<article lang="en">
<title>KEDR COI &rel-version; Reference Manual</title>
<articleinfo>
    <releaseinfo>KEDR COI Library version &rel-version; (&rel-date;)</releaseinfo>
    <authorgroup>
        <author>
            <firstname>Andrey</firstname>
            <surname>Tsyvarev</surname>
            <email>tsyvarev@ispras.ru</email>
        </author>
    </authorgroup>
</articleinfo>

<!-- ============ Top-level sections ============ -->
<!-- Introduction -->
&kedr_coi_intro;
<!-- Overview -->
&kedr_coi_overview;
<!-- Installation -->
&kedr_coi_install;
<!-- Using -->
&kedr_coi_using; 
<!-- Pre-existed interceptors -->
&pre_existed_interceptors;
<!-- Template generation of the interceptors -->
&create_interceptors_using_kedr_gen;
<!-- API Reference -->
&api_reference;

<!-- Glossary -->
&kedr_coi_glossary;


</article>
