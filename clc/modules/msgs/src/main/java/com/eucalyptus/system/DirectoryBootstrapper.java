package com.eucalyptus.system;

import org.apache.log4j.Logger;
import com.eucalyptus.bootstrap.Bootstrap;
import com.eucalyptus.bootstrap.Bootstrapper;
import com.eucalyptus.bootstrap.Component;
import com.eucalyptus.bootstrap.Provides;
import com.eucalyptus.bootstrap.RunDuring;
import com.eucalyptus.bootstrap.Bootstrap.Stage;
import com.eucalyptus.records.EventType;
import edu.ucsb.eucalyptus.msgs.EventRecord;

@Provides(Component.bootstrap)
@RunDuring(Bootstrap.Stage.UnprivilegedConfiguration)
public class DirectoryBootstrapper extends Bootstrapper {
  private static Logger LOG = Logger.getLogger( DirectoryBootstrapper.class );
  public DirectoryBootstrapper( ) {}
  @Override
  public boolean load( Stage current ) throws Exception {
    for( BaseDirectory b : BaseDirectory.values( ) ) {
      LOG.info( EventRecord.here( DirectoryBootstrapper.class, EventType.SYSTEM_DIR_CHECK, b.name(), b.toString( ) ) );
      b.check( );
    }
    for( SubDirectory s : SubDirectory.values( ) ) {
      LOG.info( EventRecord.here( DirectoryBootstrapper.class, EventType.SYSTEM_DIR_CHECK, s.name(), s.toString( ) ) );
      s.check( );
    }
    return true;
  }
  
  @Override
  public boolean start( ) throws Exception {
    return true;
  }
  
}