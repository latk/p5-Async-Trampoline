requires "perl" => "5.014";

on 'build' => sub {
  requires "Module::Build" => "0.28";
};

on 'test' => sub {
  requires "Test::Exception" => "0";
  requires "Test::More" => "0";
  requires "Test::Output" => "0";
};

on 'configure' => sub {
  requires "ExtUtils::CBuilder" => "0.280226";
  requires "Module::Build" => "0.28";
};

on 'develop' => sub {
  requires "Devel::PPPort" => "3.23";
  requires "Pod::Coverage::TrustPod" => "0";
  requires "Test::Perl::Critic" => "0";
  requires "Test::Pod" => "1.41";
  requires "Test::Pod::Coverage" => "1.08";
};
