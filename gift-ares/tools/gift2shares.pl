#!/usr/bin/perl -w
use MIME::Base64;

sub rint { unpack'L',substr $_[0],0,4,'' }
sub strnul { my ($str)=unpack "Z*",$_[0]; substr $_[0],0,1+length $str,''; $str }

local $/=undef;
$_=<>;
REC:
while ($_) {
    my $len=rint $_;
    my $rec=substr $_,0,$len,'';
    
    my $mtime=rint $rec;
    my $size=rint $rec;
    my (%hash,%meta);
    my $mime=strnul $rec;
    my $root=strnul $rec;
    my $path=strnul $rec;
    next REC if $path=~/\"\n/s;
    
    while (my $key=strnul $rec) {
	my $hlen=rint $rec;
	my $val=substr $rec,0,$hlen,'';
	die unless $key=~s/^H-//;
	$hash{$key}=$val;
    }
    while (my $key=strnul $rec) {
	my $val=strnul $rec;
	next REC if ($key.$val)=~/\"|\n/;
	$val/=1000 if $key=~/^bitrate$/;
	$meta{$key}=$val;
    }
    die if $rec;
    printf "share \"%s\" %u %u %s %s\n", 
    $path, $size, 
    {audio=>1,video=>5,image=>7,text=>6,application=>3}->{(($mime=~m[(.*)/])[0])}||'0', 
    (map {$_?substr(encode_base64($_),0,28):'-'} $hash{SHA1}),
    join' ',map {/ /?"\"$_\"":$_} %meta;
}
